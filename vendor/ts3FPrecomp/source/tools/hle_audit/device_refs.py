"""
Does a title reach its GPU only through D3D8's own functions?

    py -3 -m tools.hle_audit.device_refs games/
    py -3 -m tools.hle_audit.device_refs game/default.xbe --json refs.json
    py -3 -m tools.hle_audit.device_refs disc.iso

Replacing D3D8 by name (tools/recomp/hle.py -> src/hle) only covers what goes
through the D3D8 functions XbSymbolDatabase can name. Two things bypass that,
and both are visible in the binary:

  * Game code reading the device global itself. D3D8 keeps its device in
    D3D_g_pDevice. A reference to that address from outside D3D8 is game code
    -- often an XDK inline function compiled into the title -- touching the
    device's fields directly, where no name-keyed replacement sees it.

  * Game code filling its own push buffer. D3DDevice_BeginPush hands a title
    raw space in the NV2A command stream, and whatever it writes there skips
    the D3D8 API entirely.

For each title this counts the references to D3D_g_pDevice and the call sites
of D3DDevice_BeginPush*, and reports which lie outside D3D8.

"Inside D3D8" is decided two ways, and a site counts as outside only when both
say so:
  * by section -- non-LTCG builds link D3D8 into its own D3D, D3DX and XGRPH
    sections;
  * by name -- the address falls between a function XbSymbolDatabase named as
    D3D8 and the next named function. This is the only test left for an LTCG
    build, where D3D8 is merged into .text, and those range ends are
    approximate.

References are found by scanning the executable sections' bytes for the
address and keeping a hit only when an instruction there uses it as an
operand, so code a disassembler never discovered is still counted. Where two
overlapping decodes both fit, the one a short linear decode from further back
lands on is taken. Call sites are direct `call rel32` and `jmp rel32`.

This replaces a prose claim in src/hle/hle_d3d8.c that had no script behind
it. On Burnout 2, tools.disasm's cross-references and this scan agree on 118
references to the device global.

Symbols come from `<stem>_xdk_symbols.json` beside a loose XBE when one exists
(tools.xdk_symbols writes it), otherwise from running XbSymbolDatabaseCLI. A
title read out of a disc image always runs the CLI.
"""

import argparse
import bisect
import hashlib
import importlib.util
import json
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import capstone
from capstone import x86

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.xbe_parser.xbe_parser import SECTION_EXECUTABLE, XBEParser  # noqa: E402
from tools.xdk_symbols.__main__ import find_cli, parse_output          # noqa: E402

DEVICE_GLOBAL = "D3D_g_pDevice"
PUSH_PREFIX = "D3DDevice_BeginPush"

# Sections a non-LTCG link puts D3D8 and its helpers in.
D3D_SECTIONS = frozenset(("D3D", "D3DX", "XGRPH"))
# Libraries whose named functions count as "inside D3D8".
D3D_LIBRARIES = frozenset(("D3D8", "D3D8LTCG", "D3DX8", "XGRAPHC"))
# Libraries counted in the "named D3D8 functions" column.
D3D8_ONLY = frozenset(("D3D8", "D3D8LTCG"))


class Section:
    __slots__ = ("name", "va", "size", "raw", "raw_size", "executable")

    def __init__(self, name, va, size, raw, raw_size, executable):
        self.name = name
        self.va = va
        self.size = size
        self.raw = raw
        self.raw_size = raw_size
        self.executable = executable

    def contains(self, va):
        return self.va <= va < self.va + self.size


def sections_of(xbe):
    return [Section(s.name, s.virtual_addr, s.virtual_size, s.raw_addr,
                    s.raw_size, bool(s.flags & SECTION_EXECUTABLE))
            for s in xbe.sections]


def section_at(sections, va):
    for sec in sections:
        if sec.contains(va):
            return sec
    return None


def decoder():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    return md


def _uses(insn, target):
    for op in insn.operands:
        if op.type == x86.X86_OP_MEM and (op.mem.disp & 0xFFFFFFFF) == target:
            return True
        if op.type == x86.X86_OP_IMM and (op.imm & 0xFFFFFFFF) == target:
            return True
    return False


def _instruction_using(blob, base_va, hit, target, md):
    """Start offset of the instruction that uses `target` from the four bytes
    at `hit`, or None when no instruction does."""
    candidates = []
    for back in range(1, 12):
        start = hit - back
        if start < 0:
            break
        insn = next(md.disasm(blob[start:start + 16], base_va + start, 1), None)
        if insn is not None and insn.size >= back + 4 and _uses(insn, target):
            candidates.append(start)
    if len(candidates) <= 1:
        return candidates[0] if candidates else None

    # Overlapping decodes both fit, e.g. "8B 0D <addr>" (mov ecx, [addr]) also
    # reads as "0D <addr>" (or eax, addr) one byte later. Take the one a
    # linear decode from further back actually reaches.
    lo = max(0, hit - 64)
    boundaries = {insn.address - base_va
                  for insn in md.disasm(blob[lo:hit + 16], base_va + lo)}
    aligned = [c for c in candidates if c in boundaries]
    return aligned[0] if aligned else candidates[0]


def find_references(blob, base_va, target, md=None):
    """Addresses of instructions in `blob` that use `target` as an operand."""
    md = md or decoder()
    needle = struct.pack("<I", target & 0xFFFFFFFF)
    found = set()
    i = blob.find(needle)
    while i >= 0:
        start = _instruction_using(blob, base_va, i, target, md)
        if start is not None:
            found.add(base_va + start)
        i = blob.find(needle, i + 1)
    return sorted(found)


def find_calls(blob, base_va, target):
    """Addresses of direct `call rel32` / `jmp rel32` instructions to `target`.

    Not confirmed by decoding: an E8 or E9 byte whose next four bytes happen
    to land exactly on a named function is a one-in-four-billion coincidence
    per byte."""
    found = []
    for opcode in (0xE8, 0xE9):
        byte = bytes((opcode,))
        i = blob.find(byte)
        while 0 <= i <= len(blob) - 5:
            rel = struct.unpack_from("<i", blob, i + 1)[0]
            if (base_va + i + 5 + rel) & 0xFFFFFFFF == target:
                found.append(base_va + i)
            i = blob.find(byte, i + 1)
    return sorted(found)


def named_ranges(symbols, sections, libraries=D3D_LIBRARIES):
    """[start, end) for each function named in `libraries`, ending at the
    next named function of any library or at the end of its section."""
    starts = sorted({s["address"] for s in symbols if s.get("kind") == "function"})
    ranges = []
    for s in symbols:
        if s.get("kind") != "function" or s.get("library") not in libraries:
            continue
        sec = section_at(sections, s["address"])
        if sec is None:
            continue
        i = bisect.bisect_right(starts, s["address"])
        end = starts[i] if i < len(starts) else sec.va + sec.size
        ranges.append((s["address"], min(end, sec.va + sec.size)))
    ranges.sort()
    return ranges


def in_ranges(ranges, va):
    i = bisect.bisect_right(ranges, (va, 0xFFFFFFFFFF)) - 1
    return i >= 0 and ranges[i][0] <= va < ranges[i][1]


def classify(sites, sections, ranges):
    by_section = by_name = 0
    outside = []
    for va in sites:
        sec = section_at(sections, va)
        in_section = sec is not None and sec.name in D3D_SECTIONS
        in_name = in_ranges(ranges, va)
        by_section += in_section
        by_name += in_name
        if not in_section and not in_name:
            outside.append(va)
    return {
        "total": len(sites),
        "inside_by_section": by_section,
        "inside_by_name": by_name,
        "outside": len(outside),
        "outside_sites": ["0x%08X" % va for va in outside],
    }


def parse_bytes(xbe_bytes):
    """XBEParser reads from a path, so bytes go via a temporary file."""
    handle, tmp = tempfile.mkstemp(suffix=".xbe")
    try:
        with os.fdopen(handle, "wb") as f:
            f.write(xbe_bytes)
        return XBEParser(tmp).parse()
    finally:
        os.unlink(tmp)


def verdict(report):
    if report.get("error"):
        return "not measured: " + report["error"]
    if report.get("device_refs") is None:
        return "not measured: %s is not named for this build" % DEVICE_GLOBAL
    problems = []
    if report["device_refs"]["outside"]:
        problems.append("%d device-global reference(s) outside D3D8"
                        % report["device_refs"]["outside"])
    if not report["begin_push"]["functions"]:
        problems.append("BeginPush not named, push buffers unmeasured")
    elif report["begin_push"]["outside"]:
        problems.append("%d BeginPush call site(s) outside D3D8"
                        % report["begin_push"]["outside"])
    text = "; ".join(problems) if problems else "D3D8 boundary holds"
    if report.get("ltcg"):
        text += " (LTCG: judged by name only)"
    return text


def measure(xbe_bytes, symbols, label=""):
    """Measure one title. `symbols` is XbSymbolDatabase output in the form
    tools.xdk_symbols writes: dicts with library, kind, address and name."""
    xbe = parse_bytes(xbe_bytes)
    libraries = [(lib.name.strip("\x00 "), lib.build) for lib in xbe.libraries]
    d3d = next(((n, b) for n, b in libraries if n.upper().startswith("D3D8")), None)

    report = {
        "label": str(label),
        "title": xbe.certificate.title_name.strip("\x00 "),
        "title_id": "0x%08X" % xbe.certificate.title_id,
        "xdk": d3d[1] if d3d else max((b for _, b in libraries), default=0),
        "d3d_library": d3d[0] if d3d else "",
        "ltcg": bool(d3d and "LTCG" in d3d[0].upper()),
        "d3d8_functions_named": sum(1 for s in symbols if s.get("kind") == "function"
                                    and s.get("library") in D3D8_ONLY),
    }

    sections = sections_of(xbe)
    ranges = named_ranges(symbols, sections)
    device = next((s["address"] for s in symbols if s.get("name") == DEVICE_GLOBAL), None)
    push = [s for s in symbols if s.get("kind") == "function"
            and str(s.get("name", "")).startswith(PUSH_PREFIX)]

    md = decoder()
    refs, calls = [], []
    raw = xbe.raw_data
    for sec in sections:
        if not sec.executable or not sec.raw_size:
            continue
        blob = raw[sec.raw:sec.raw + sec.raw_size]
        if device is not None:
            refs += find_references(blob, sec.va, device, md)
        for fn in push:
            calls += find_calls(blob, sec.va, fn["address"])

    report["device_global"] = None if device is None else "0x%08X" % device
    report["device_refs"] = None if device is None else classify(refs, sections, ranges)
    report["begin_push"] = dict(functions=sorted(fn["name"] for fn in push),
                                **classify(sorted(set(calls)), sections, ranges))
    report["verdict"] = verdict(report)
    return report


def symbols_from_cli(xbe_path, cli):
    if not cli:
        raise RuntimeError("XbSymbolDatabaseCLI not found; pass --cli or see "
                           "py -3 -m tools.xdk_symbols --help")
    run = subprocess.run([cli, str(xbe_path), "-e"], capture_output=True, text=True)
    if run.returncode != 0:
        raise RuntimeError("%s exited %d: %s" % (cli, run.returncode, run.stderr.strip()[:300]))
    return parse_output(run.stdout)


def symbols_for(label, xbe_bytes, loose_path, cli, fresh):
    """(symbols, source) for one title."""
    if loose_path is not None:
        beside = loose_path.with_name(loose_path.stem + "_xdk_symbols.json")
        if not fresh and beside.is_file():
            return json.loads(beside.read_text(encoding="utf-8"))["symbols"], "cached"
        return symbols_from_cli(loose_path, cli), "cli"
    handle, tmp = tempfile.mkstemp(suffix=".xbe")
    try:
        with os.fdopen(handle, "wb") as f:
            f.write(xbe_bytes)
        return symbols_from_cli(tmp, cli), "cli"
    finally:
        os.unlink(tmp)


def _survey():
    """The survey script's disc discovery: loose XBEs and default.xbe inside
    .iso/.xiso images."""
    path = ROOT / "scripts" / "survey_xbe_library.py"
    spec = importlib.util.spec_from_file_location("survey_xbe_library", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def print_table(reports, out=sys.stdout):
    head = "%-30s %5s %-8s %5s %8s %7s %6s %6s  %s"
    out.write(head % ("TITLE", "XDK", "D3D", "NAMED", "DEV REFS", "OUTSIDE",
                      "PUSH", "P.OUT", "VERDICT") + "\n")
    for r in reports:
        label = Path(r["label"])
        name = (r.get("title")
                or (label.name if label.suffix.lower() in (".iso", ".xiso") else label.parent.name)
                or r["label"])[:30]
        if "d3d8_functions_named" not in r:
            out.write("%-30s %s\n" % (name, r["verdict"]))
            continue
        dev = r["device_refs"] or {}
        push = r["begin_push"]
        out.write(head % (name, r["xdk"], r["d3d_library"] or "-",
                          r["d3d8_functions_named"], dev.get("total", "-"),
                          dev.get("outside", "-"), push["total"], push["outside"],
                          r["verdict"]) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.strip().split("\n\n")[0])
    ap.add_argument("paths", nargs="+",
                    help="default.xbe files, disc images, or folders to search")
    ap.add_argument("--json", help="also write the full reports to this file")
    ap.add_argument("--cli", help="path to XbSymbolDatabaseCLI")
    ap.add_argument("--fresh", action="store_true",
                    help="ignore <stem>_xdk_symbols.json beside an XBE and run the CLI")
    args = ap.parse_args(argv)

    survey = _survey()
    cli = find_cli(args.cli)
    reports, seen = [], {}
    for root in args.paths:
        for path, data in survey.find_targets(Path(root)):
            label = str(path)
            if isinstance(data, survey._Unreadable):
                reports.append({"label": label, "error": data.reason,
                                "verdict": "not measured: " + data.reason})
                continue
            raw = Path(path).read_bytes() if data is None else data
            digest = hashlib.sha1(raw).hexdigest()
            if digest in seen:
                reports.append({"label": label, "duplicate_of": seen[digest],
                                "verdict": "same XBE as " + seen[digest]})
                continue
            seen[digest] = label
            try:
                symbols, source = symbols_for(label, raw, Path(path) if data is None else None,
                                              cli, args.fresh)
                report = measure(raw, symbols, label)
                report["symbols_source"] = source
            except Exception as exc:                      # noqa: BLE001
                report = {"label": label, "error": "%s: %s" % (type(exc).__name__, exc)}
                report["verdict"] = verdict(report)
            reports.append(report)

    print_table(reports)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(reports, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
