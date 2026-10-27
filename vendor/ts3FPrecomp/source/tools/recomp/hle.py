"""
Replace XDK library functions by name instead of lifting them.

The DOAXBV port reached gameplay by replacing D3D8 at its API and translating
to D3D11, never emulating the GPU underneath. It found each D3D8 function by a
hard-coded address, which makes the replacement one game's. Here the address
comes from tools.xdk_symbols (XbSymbolDatabase signatures), and the
replacement is keyed by the XDK function's *name*, so one implementation of
D3DDevice_SetTexture serves every title whose XDK build the database covers.

Mechanism, reusing what the recompiler already does for hand-written code:

  * an implemented name's address joins the `manual` set, so its body is not
    lifted and every direct call routes through the runtime lookup;
  * a generated `recomp_hle.c` defines that address's `sub_XXXXXXXX` as a
    thunk that calls `hle_<Name>` and then pops the stack.

The thunk, not the implementation, pops the stack, and the count comes from
the original function's own `ret N`. The signature database's parameter lists
are right for 311 of the 314 D3D8 and DirectSound functions checked against
the lifted code in Burnout 2 -- and wrong for three, which would leave esp off
in every caller. The binary cannot be wrong about what it pops.

Implementations mark themselves with `HLE_EXPORT(Name)`; this module scans for
that marker the way manual_scan scans recomp_manual.c, so the list of what is
replaced cannot drift from what is written.
"""

import glob
import json
import os
import re

EXPORT_RE = re.compile(r"^\s*HLE_EXPORT\(\s*([A-Za-z_]\w*)\s*\)", re.M)

# A replacement that needs an XDK *variable's* address marks it
# HLE_IMPORT_VAR(Name); the generated recomp_hle.c defines hle_var_<Name>.
# XAPI's input functions are the first user: the device type a title passes
# (g_DeviceType_Gamepad, g_DeviceType_MU) is a pointer into the title's own
# data, so which call is about a pad can only be answered by name.
IMPORT_VAR_RE = re.compile(r"^\s*HLE_IMPORT_VAR\(\s*([A-Za-z_]\w*)\s*\)", re.M)


def implemented_names(paths):
    """Names marked HLE_EXPORT(...) in the given C files or directories."""
    names = set()
    for path in paths:
        files = (glob.glob(os.path.join(path, "**", "*.c"), recursive=True)
                 if os.path.isdir(path) else [path])
        for f in files:
            with open(f, encoding="utf-8", errors="replace") as fh:
                names.update(EXPORT_RE.findall(fh.read()))
    return names


def imported_variables(paths):
    """Names marked HLE_IMPORT_VAR(...) in the given C files or directories."""
    names = set()
    for path in paths:
        files = (glob.glob(os.path.join(path, "**", "*.c"), recursive=True)
                 if os.path.isdir(path) else [path])
        for f in files:
            with open(f, encoding="utf-8", errors="replace") as fh:
                names.update(IMPORT_VAR_RE.findall(fh.read()))
    return names


def load_symbols(path):
    """Function symbols from a tools.xdk_symbols JSON file."""
    with open(path, encoding="utf-8") as fh:
        data = json.load(fh)
    return [s for s in data.get("symbols", []) if s.get("kind") == "function"]


def load_variables(path):
    """Variable symbols from a tools.xdk_symbols JSON file."""
    with open(path, encoding="utf-8") as fh:
        data = json.load(fh)
    return [s for s in data.get("symbols", []) if s.get("kind") == "variable"]


def resolve_variables(names, symbols):
    """Map each imported name to its address, or 0 when it cannot be named.

    `symbols` may hold variables and functions alike: an implementation that
    needs to know where an XDK *function* sits in the title -- to read its
    code, not to call it -- imports it the same way. The D3D replacement reads
    the device-struct offsets out of D3D_BlockOnTime's prologue this way,
    because they move between XDK builds and the bytes are the one place they
    are written down.

    The database can list one symbol twice at the same address; that is one
    answer. Two different addresses for a name would be a guess, so it gets 0
    and a note. Returns (values, notes); every name is in `values`, so the
    generated file always defines what the implementations declare.
    """
    by_name = {}
    for s in symbols:
        by_name.setdefault(s["name"], set()).add(s["address"])
    values, notes = {}, []
    for name in sorted(names):
        addrs = by_name.get(name, set())
        if len(addrs) == 1:
            values[name] = next(iter(addrs))
            continue
        values[name] = 0
        notes.append(f"import {name}: " + (
            "not found in this XBE" if not addrs else
            f"{len(addrs)} different addresses, left 0"))
    return values, notes


def stack_cleanup(code, start_va):
    """Bytes of arguments a function's `ret` pops, or None if unknowable.

    Linear sweep over the function's bytes. Every `ret` must agree: a
    function with two different counts, or no `ret` at all (it leaves only by
    tail jumps), has no single number a thunk could use.
    """
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    pops = set()
    for insn in md.disasm(code, start_va):
        if insn.mnemonic == "ret":
            pops.add(int(insn.op_str, 0) if insn.op_str else 0)
    return pops.pop() if len(pops) == 1 else None


# Link-time code generation renames the functions it rewrites.
#
# When a title is built with LTCG the linker is free to change a function's
# calling convention, and the signature database records what it did in the
# name: D3DDevice_LoadVertexShader_4__LTCG_eax1 is LoadVertexShader with four
# bytes of stack arguments and its first argument in eax, and
# D3DDevice_SelectVertexShader_0__LTCG_eax1_ebx2 takes both of its arguments
# in registers and none on the stack.
#
# Matching only the plain name meant none of these was ever replaced. Black is
# the first LTCG title here and carries fifteen of them, including the whole
# vertex-shader path -- which is precisely what had to be replaced to make
# TimeSplitters 2 draw.
LTCG_RE = re.compile(r"^(?P<base>.+?)_(?P<stack>\d+)__LTCG_(?P<regs>[a-z0-9_]+)$")
_LTCG_REG_RE = re.compile(r"(e[a-z]{2})(\d+)")


def parse_ltcg(name):
    """(base name, {1-based argument position: register}) or (name, {})."""
    m = LTCG_RE.match(name)
    if not m:
        return name, {}
    regs = {int(pos): reg for reg, pos in _LTCG_REG_RE.findall(m.group("regs"))}
    return m.group("base"), regs


def plan(symbols, implemented, known_addrs, manual, cleanup):
    """Decide which addresses to replace.

    `cleanup(addr)` returns the argument bytes that function's `ret` pops, or
    None. Returns (replace, notes): `replace` maps address ->
    (name, pop, regs), where regs maps a 1-based argument position to the
    register that carries it in an LTCG build and is empty otherwise;
    `notes` lists why an implemented name was not used, so a silent miss is
    visible.
    """
    replace, notes = {}, []
    by_name, by_base = {}, {}
    for sym in symbols:
        by_name.setdefault(sym["name"], []).append(sym["address"])
        base, regs = parse_ltcg(sym["name"])
        if regs:
            by_base.setdefault(base, []).append((sym["address"], regs,
                                                 sym["name"]))
    for name in sorted(implemented):
        addrs = by_name.get(name)
        regs = {}
        if not addrs:
            # No plain match. An LTCG build renames what it rewrites, so look
            # for exactly one variant of this name before giving up.
            variants = by_base.get(name, [])
            if len(variants) == 1:
                addr, regs, full = variants[0]
                addrs = [addr]
                notes.append(f"{name}: matched {full} (LTCG, "
                             f"{len(regs)} register argument"
                             f"{'' if len(regs) == 1 else 's'})")
            elif len(variants) > 1:
                notes.append(f"{name}: {len(variants)} LTCG variants, skipped")
                continue
        if not addrs:
            notes.append(f"{name}: not found in this XBE")
            continue
        if len(addrs) > 1:
            # Two addresses for one name means the signature matched twice;
            # replacing either would be a guess.
            notes.append(f"{name}: {len(addrs)} matches, skipped")
            continue
        addr = addrs[0]
        if addr in manual:
            notes.append(f"{name}: 0x{addr:08X} is hand-written in the title, "
                         "which wins")
            continue
        if addr not in known_addrs:
            notes.append(f"{name}: 0x{addr:08X} is not a detected function")
            continue
        pop = cleanup(addr)
        if pop is None:
            notes.append(f"{name}: 0x{addr:08X} has no single `ret N`, skipped")
            continue
        replace[addr] = (name, pop, regs)
    return replace, notes


# A replacement that also needs the original body -- to run it and then do
# its own work -- marks it HLE_ORIGINAL(Name). The body is still lifted, as
# sub_XXXXXXXX_hle_original, and the generated recomp_hle.c points
# hle_original_<Name> at it. The address itself stays replaced, so every call
# keeps reaching hle_<Name>. This is how a host renderer can be brought up
# beside the title's own D3D8 without changing what the title sees.
ORIGINAL_RE = re.compile(r"^\s*HLE_ORIGINAL\(\s*([A-Za-z_]\w*)\s*\)", re.M)
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.S)


def wanted_originals(paths):
    """Names marked HLE_ORIGINAL(...) in the given C files or directories.

    Block comments are removed first, so a marker quoted on its own line in a
    comment is not taken for a real one. Preprocessor conditionals are not
    evaluated: a marker under #if 0 still counts, and costs one kept body."""
    names = set()
    for path in paths:
        files = (glob.glob(os.path.join(path, "**", "*.c"), recursive=True)
                 if os.path.isdir(path) else [path])
        for f in files:
            with open(f, encoding="utf-8", errors="replace") as fh:
                text = BLOCK_COMMENT_RE.sub("", fh.read())
            names.update(ORIGINAL_RE.findall(text))
    return names


def original_name(addr):
    """The name a kept original body is lifted under."""
    return f"sub_{addr:08X}_hle_original"


def keep_originals(replace, wanted):
    """{address: body name} for replaced functions whose original is wanted.

    Only replaced addresses qualify. An unreplaced function is lifted under
    its own name anyway, and needs nothing kept."""
    return {addr: original_name(addr)
            for addr, (name, _, _r) in replace.items() if name in wanted}


def render_thunks(replace, variables=None, originals=None):
    """C source defining each replaced sub_XXXXXXXX as a thunk to hle_<Name>.

    The thunk pops the return address the caller pushed plus the argument
    bytes the original `ret N` popped, exactly as the lifted body would have.
    `variables` maps imported XDK variable names to addresses (0 = unknown);
    each becomes `const uint32_t hle_var_<Name>`. `originals` maps every name
    marked HLE_ORIGINAL to the address whose body was kept, or None when this
    title does not replace it; each becomes `hle_original_<Name>` (0 = none).
    """
    lines = [
        "/* Generated by tools.recomp from the title's XDK symbols.",
        " * Each XDK function below is replaced by name: the lifted body is not",
        " * generated, and the address's sub_XXXXXXXX calls hle_<Name>, then",
        " * pops what the original function's own `ret N` popped. */",
        '#include "recomp_types.h"',
        "",
    ]
    for addr, (name, _, _r) in sorted(replace.items()):
        lines.append(f"void hle_{name}(void);")
    lines.append("")
    for addr, (name, pop, regs) in sorted(replace.items()):
        # g_esp, not esp: `esp` is a shorthand local to each lifted chunk,
        # while recomp_types.h declares the real register for every file.
        if not regs:
            lines.append(f"void sub_{addr:08X}(void) {{ hle_{name}(); "
                         f"g_esp += {4 + pop}; }}"
                         f"  /* {name}, ret {pop} */")
            continue

        # An LTCG variant takes some arguments in registers. The
        # implementation is written against the ordinary convention and reads
        # everything with HLE_ARG, so the thunk lays a normal argument frame
        # out below the stack and points g_esp at it for the duration of the
        # call. Registers go to the positions their name records; the rest
        # come off the caller's stack in order.
        total = pop // 4 + len(regs)
        fill = []
        stack_i = 0
        for pos in range(1, total + 1):
            if pos in regs:
                src = "g_" + regs[pos]
            else:
                src = f"RECOMP_HLE_STACK({stack_i})"
                stack_i += 1
            fill.append(f"a[{pos - 1}] = {src};")
        lines.append(
            f"void sub_{addr:08X}(void) {{ RECOMP_HLE_LTCG_CALL({total}, "
            f"hle_{name}, {4 + pop}) {{ {' '.join(fill)} }} "
            f"RECOMP_HLE_LTCG_END }}"
            f"  /* {name}, ret {pop}, "
            + ", ".join(f"arg{p} in {r}" for p, r in sorted(regs.items()))
            + " */")
    lines.append("")
    if variables:
        lines.append("/* XDK variables the replacements import by name "
                     "(0 = not found). */")
        for name, addr in sorted(variables.items()):
            lines.append(f"const uint32_t hle_var_{name} = 0x{addr:08X}u;")
        lines.append("")
    if originals:
        lines.append("/* Original bodies the replacements run first "
                     "(0 = not replaced in this title). */")
        for name, addr in sorted(originals.items()):
            if addr is None:
                lines.append(f"void (*const hle_original_{name})(void) = 0;")
            else:
                lines.append(f"void {original_name(addr)}(void);")
                lines.append(f"void (*const hle_original_{name})(void) = "
                             f"{original_name(addr)};")
        lines.append("")
    return "\n".join(lines)
